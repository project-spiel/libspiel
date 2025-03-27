#include <spiel.h>

static void
registry_get_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_autoptr (GError) err = NULL;
  g_autoptr (GListModel) providers = NULL;
  g_autoptr (GListModel) voices = NULL;
  g_autoptr (SpielRegistry) registry = spiel_registry_get_finish (result, &err);

  g_assert_no_error (err);
  g_assert (registry != NULL);

  providers = spiel_registry_get_providers (registry);
  g_assert (g_list_model_get_n_items (providers) == 3);

  voices = spiel_registry_get_voices (registry);
  g_assert (g_list_model_get_n_items (voices) == 8);

  g_main_loop_quit (loop);
}

static void
test_registry (void)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);

  spiel_registry_get (NULL, registry_get_cb, loop);
  g_main_loop_run (loop);
}

gint
main (gint argc, gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/spiel/test_registry", test_registry);
  return g_test_run ();
}