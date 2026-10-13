// A successful link alone does not guarantee that a dynamically loaded DLL
// exports the OpenXR entry points SDL needs. No installed XR runtime required.
#include <SDL3/SDL.h>
#include <cstdio>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    SDL_SharedObject *loader=SDL_LoadObject(argv[1]);
    if (!loader) { std::fprintf(stderr,"%s\n",SDL_GetError()); return 1; }
    const char *names[]={"xrGetInstanceProcAddr", "xrEnumerateApiLayerProperties",
                         "xrEnumerateInstanceExtensionProperties", "xrCreateInstance"};
    bool ok=true;
    for (const char *name:names) {
        if (!SDL_LoadFunction(loader,name)) {
            std::fprintf(stderr,"Missing loader export %s: %s\n",name,SDL_GetError());
            ok=false;
        }
    }
    SDL_UnloadObject(loader);
    return ok ? 0 : 1;
}
