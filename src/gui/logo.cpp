
#include "logo.h"
#include "logo_data.h"

GdkPixbuf *logo_pixbuf() {
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    if (!loader) return nullptr;

    GError *error = nullptr;
    bool ok = gdk_pixbuf_loader_write(loader, iruka_logo_png, iruka_logo_png_len, &error);
    if (ok) ok = gdk_pixbuf_loader_close(loader, &error);

    GdkPixbuf *pixbuf = nullptr;
    if (ok) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) g_object_ref(pixbuf);
    } else {
        g_warning("Failed to decode embedded logo: %s",
                  error ? error->message : "unknown error");
        if (error) g_error_free(error);
    }

    g_object_unref(loader);
    return pixbuf;
}
