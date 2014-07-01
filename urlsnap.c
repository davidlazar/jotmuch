// urlsnap: save snapshots of webpages
// Copyright (C) 2014 David Lazar
#include <err.h>
#include <glib/gstdio.h>
#include <webkit2/webkit2.h>

static char *mhtml_file = NULL;
static char *html_file = NULL;
static char *png_file = NULL;
static char *title_file = NULL;

static int todo = 0;

static GOptionEntry entries[] = {
    { "mhtml", 0, 0, G_OPTION_ARG_FILENAME, &mhtml_file, "Write MHTML snapshot to FILE", "FILE" },
    { "html", 0, 0, G_OPTION_ARG_FILENAME, &html_file, "Write HTML snapshot to FILE", "FILE" },
    { "png", 0, 0, G_OPTION_ARG_FILENAME, &png_file, "Write PNG snapshot to FILE", "FILE" },
    { "title", 0, 0, G_OPTION_ARG_FILENAME, &title_file, "Write page title to FILE", "FILE" },
    { NULL }
};

static void done() {
    todo--;
    if (todo == 0) {
        gtk_main_quit();
    }
}

static void mhtml_finished(GObject *object, GAsyncResult *result, gpointer user_data) {
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(object);
    GError *error = NULL;
    gboolean ok = webkit_web_view_save_to_file_finish(web_view, result, &error);
    if (ok == FALSE) {
        errx(1, "error saving mhtml: %s", error->message);
    }
    done();
}

static void html_finished(GObject *object, GAsyncResult *result, gpointer user_data) {
    WebKitWebResource *wr = WEBKIT_WEB_RESOURCE(object);
    GError *error = NULL;
    gsize length;
    guchar *h = webkit_web_resource_get_data_finish(wr, result, &length, &error);
    FILE *f = g_fopen(html_file, "w");
    if (fwrite(h, length, 1, f) != 1) {
        errx(1, "error saving html");
    }
    fclose(f);
    done();
}

static void png_finished(GObject *object, GAsyncResult *result, gpointer user_data) {
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(object);
    GError *error = NULL;
    cairo_surface_t *surface = webkit_web_view_get_snapshot_finish(web_view, result, &error);
    if (surface == NULL) {
        errx(1, "error creating snapshot: %s", error->message);
    }
    cairo_surface_write_to_png(surface, png_file);
    cairo_surface_destroy(surface);
    done();
}

static void load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data) {
    if (load_event != WEBKIT_LOAD_FINISHED) {
        return;
    }

    if (mhtml_file != NULL) {
        GFile *f = g_file_new_for_path(mhtml_file);
        webkit_web_view_save_to_file(web_view, f, WEBKIT_SAVE_MODE_MHTML, NULL, mhtml_finished, NULL);
    }

    if (html_file != NULL) {
        WebKitWebResource *wr = webkit_web_view_get_main_resource(web_view);
        webkit_web_resource_get_data(wr, NULL, html_finished, NULL);
    }

    if (png_file != NULL) {
        webkit_web_view_get_snapshot(
            web_view,
            WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
            WEBKIT_SNAPSHOT_OPTIONS_NONE,
            NULL,
            png_finished,
            NULL
        );
    }

    if (title_file != NULL) {
        const gchar *title = webkit_web_view_get_title(web_view);
        GString *s = g_string_new(title);
        g_string_append_c(s, '\n');
        FILE *f = g_fopen(title_file, "w");
        if (fwrite(s->str, s->len, 1, f) != 1) {
            errx(1, "error saving title");
        }
        fclose(f);
        done();
    }
}

static void load_failed(WebKitWebView *web_view, WebKitLoadEvent load_event,
  gchar *failing_uri, gpointer error, gpointer user_data) {
    errx(1, "load failed: %s: %s", ((GError *) error)->message, failing_uri);
}

int main(int argc, gchar* argv[]) {
    gtk_init(&argc, &argv);

    GError *error = NULL;
    GOptionContext *context;
    context = g_option_context_new("URL");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        errx(1, "option parsing failed: %s", error->message);
    }

    if (argc < 2) {
        printf("Usage: %s [OPTION...] URL\n", argv[0]);
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    todo = (mhtml_file != NULL) + (html_file != NULL) + (png_file != NULL) + (title_file != NULL);
    if (!todo) {
        printf("Specify at least one of: --mhtml, --html, --png, --title\n");
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    const gchar *url = argv[1];
    if (g_uri_parse_scheme(url) == NULL) {
        errx(1, "invalid URI: %s", url);
    }

    WebKitWebView *web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), NULL);
    g_signal_connect(web_view, "load-failed", G_CALLBACK(load_failed), NULL);

    GtkWidget *window = gtk_offscreen_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 1024);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(web_view));
    gtk_widget_show_all(window);

    webkit_web_view_load_uri(web_view, url);
    gtk_main();

    return 0;
}
