/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

#include <gjs/gjs.h>
#include <gjs/coverage.h>
#include <gjs/debugger.h>

static gboolean debugger_enabled = FALSE;
static gboolean debugger_suspended = FALSE;
static gboolean debugger_continuation = FALSE;
static char *debugger_host = NULL;
static gint debugger_port = 8089;
static char **include_path = NULL;
static char **coverage_paths = NULL;
static char *coverage_output_path = NULL;
static char *command = NULL;

static GOptionEntry entries[] = {
    { "command", 'c', 0, G_OPTION_ARG_STRING, &command, "Program passed in as a string", "COMMAND" },
    { "coverage-path", 'C', 0, G_OPTION_ARG_STRING_ARRAY, &coverage_paths, "Add the filename FILE to the list of files to generate coverage info for", "FILE" },
    { "coverage-output", 0, 0, G_OPTION_ARG_STRING, &coverage_output_path, "Write coverage output to a directory DIR. This option is mandatory when using --coverage-path", "DIR", },
    { "include-path", 'I', 0, G_OPTION_ARG_STRING_ARRAY, &include_path, "Add the directory DIR to the list of directories to search for js files.", "DIR" },
    { "debugger", 'D', 0, G_OPTION_ARG_NONE, &debugger_enabled, "Enables the JS remote debugger.", NULL },
    { "debugger-suspended", 'S', 0, G_OPTION_ARG_NONE, &debugger_suspended, "Starts debugger in suspended state.", NULL },
    { "debugger-continuation", 'R', 0, G_OPTION_ARG_NONE, &debugger_continuation, "Continues execution when all clients have disconnected.", NULL },
    { "debugger-host", 'H', 0, G_OPTION_ARG_STRING, &debugger_host, "A host the debugger should be bound to.", NULL },
    { "debugger-port", 'P', 0, G_OPTION_ARG_INT, &debugger_port, "A post the debugger should be bound to.", NULL },
    { NULL }
};

G_GNUC_NORETURN
static void
print_help (GOptionContext *context,
            gboolean        main_help)
{
  gchar *help;

  help = g_option_context_get_help (context, main_help, NULL);
  g_print ("%s", help);
  g_free (help);

  exit (0);
}

int
main(int argc, char **argv)
{
    GOptionContext *context;
    GError *error = NULL;
    GjsContext *js_context;
    GjsDebugger *js_debugger = NULL;
    GjsCoverage *coverage = NULL;
    char *script;
    const char *filename;
    const char *program_name;
    gsize len;
    int code;

    context = g_option_context_new(NULL);

    /* pass unknown through to the JS script */
    g_option_context_set_ignore_unknown_options(context, TRUE);
    g_option_context_set_help_enabled(context, FALSE);

    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error))
        g_error("option parsing failed: %s", error->message);

    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
            print_help(context, TRUE);
        else if (strcmp(argv[1], "--help-all") == 0)
            print_help(context, FALSE);
    }

    g_option_context_free (context);

    setlocale(LC_ALL, "");

    if (command != NULL) {
        script = command;
        len = strlen(script);
        filename = "<command line>";
        program_name = argv[0];
    } else if (argc <= 1) {
        script = g_strdup("const Console = imports.console; Console.interact();");
        len = strlen(script);
        filename = "<stdin>";
        program_name = argv[0];
    } else /*if (argc >= 2)*/ {
        error = NULL;
        if (!g_file_get_contents(argv[1], &script, &len, &error)) {
            g_printerr("%s\n", error->message);
            exit(1);
        }
        filename = argv[1];
        program_name = argv[1];
        argc--;
        argv++;
    }

    js_context = (GjsContext*) g_object_new(GJS_TYPE_CONTEXT,
                                            "search-path", include_path,
                                            "program-name", program_name,
                                            NULL);

    if( debugger_enabled ) {

        js_debugger = (GjsDebugger*) g_object_new(GJS_TYPE_DEBUGGER,
                                                  "host", debugger_host,
                                                  "port", debugger_port,
                                                  NULL);

        GjsDebuggerEngineOptions options;
        options.continuation = debugger_continuation;
        options.suspend = debugger_suspended;
        options.source_displacement = -1;

        if( !gjs_debugger_install( js_debugger, js_context, "gjs-console", &options, &error ) ) {
            g_printerr("Failed to install debugger for JSContext: %s\n", error->message);
            g_clear_error(&error);
            goto out;
        }

        if( !gjs_debugger_start( js_debugger, &error ) ) {
            g_printerr("Failed to start JS debugger: %s\n", error->message);
            g_clear_error(&error);
            goto out;
        }

        g_print("Debugger is listening on port: %d\n", debugger_port);
        if( debugger_suspended ) {
            g_print("Application is suspended.\n");
        }

    }

    if (coverage_paths) {
        if (!coverage_output_path)
            g_error("--coverage-output is required when taking coverage statistics");

        coverage = gjs_coverage_new((const gchar **) coverage_paths,
                                    js_context);
    }

    /* prepare command line arguments */
    if (!gjs_context_define_string_array(js_context, "ARGV",
                                         argc - 1, (const char**)argv + 1,
                                         &error)) {
        code = 1;
        g_printerr("Failed to defined ARGV: %s", error->message);
        g_clear_error(&error);
        goto out;
    }

    /* evaluate the script */
    if (!gjs_context_eval(js_context, script, len,
                          filename, &code, &error)) {
        code = 1;
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        goto out;
    }

 out:

    /* Probably doesn't make sense to write statistics on failure */
    if (coverage && code == 0)
         gjs_coverage_write_statistics(coverage,
                                       coverage_output_path);

    if( debugger_enabled ) {
        gjs_debugger_stop( js_debugger, &error );
        gjs_debugger_uninstall( js_debugger, js_context, &error );
        g_object_unref(js_debugger);
    }
 
    g_object_unref(js_context);
    g_free(script);
    exit(code);
}
