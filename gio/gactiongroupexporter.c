/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2011 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gactiongroupexporter.h"

#include "gdbusmethodinvocation.h"
#include "gdbusintrospection.h"
#include "gdbusconnection.h"
#include "gactiongroup.h"
#include "gapplication.h"
#include "gdbuserror.h"

/**
 * SECTION:gactiongroupexporter
 * @title: GActionGroup exporter
 * @short_description: Export GActionGroups on D-Bus
 * @see_also: #GActionGroup, #GDBusActionGroup
 *
 * These functions support exporting a #GActionGroup on D-Bus.
 * The D-Bus interface that is used is a private implementation
 * detail.
 *
 * To access an exported #GActionGroup remotely, use
 * g_dbus_action_group_new() to obtain a #GDBusActionGroup.
 */

G_GNUC_INTERNAL GVariant *
g_action_group_describe_action (GActionGroup *action_group,
                                const gchar  *name)
{
  const GVariantType *type;
  GVariantBuilder builder;
  gboolean enabled;
  GVariant *state;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(bgav)"));

  enabled = g_action_group_get_action_enabled (action_group, name);
  g_variant_builder_add (&builder, "b", enabled);

  if ((type = g_action_group_get_action_parameter_type (action_group, name)))
    {
      gchar *str = g_variant_type_dup_string (type);
      g_variant_builder_add (&builder, "g", str);
      g_free (str);
    }
  else
    g_variant_builder_add (&builder, "g", "");

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("av"));
  if ((state = g_action_group_get_action_state (action_group, name)))
    {
      g_variant_builder_add (&builder, "v", state);
      g_variant_unref (state);
    }
  g_variant_builder_close (&builder);

  return g_variant_builder_end (&builder);
}

/* The org.gtk.Actions interface
 * =============================
 *
 * This interface describes a group of actions.
 *
 * Each action:
 * - has a unique string name
 * - can be activated
 * - optionally has a parameter type that must be given to the activation
 * - has an enabled state that may be true or false
 * - optionally has a state which can change value, but not type
 *
 * Methods
 * -------
 *
 * List :: () → (as)
 *
 *   Lists the names of the actions exported at this object path.
 *
 * Describe :: (s) → (bgav)
 *
 *   Describes a single action, or a given name.
 *
 *   The return value has the following components:
 *   b: specifies if the action is currently enabled. This is
 *      a hint that attempting to interact with the action will
 *      produce no effect.
 *   g: specifies the optional parameter type. If not "",
 *      the string specifies the type of argument that must
 *      be passed to the activation.
 *   av: specifies the optional state. If not empty, the array
 *       contains the current value of the state as a variant
 *
 * DescribeAll :: () → (a{s(bgav)})
 *
 *   Describes all actions in a single round-trip.
 *
 *   The dictionary maps action name strings to their descriptions
 *   (in the format discussed above).
 *
 * Activate :: (sava{sv}) → ()
 *
 *   Requests activation of the named action.
 *
 *   The action is named by the first parameter (s).
 *
 *   If the action activation requires a parameter then this parameter
 *   must be given in the second parameter (av). If there is no parameter
 *   to be specified, the array must be empty.
 *
 *   The final parameter (a{sv}) is a list of "platform data".
 *
 *   This method is not guaranteed to have any particular effect. The
 *   implementation may take some action (including changing the state
 *   of the action, if it is stateful) or it may take none at all. In
 *   particular, callers should expect their request to be completely
 *   ignored when the enabled flag is false (but even this is not
 *   guaranteed).
 *
 * SetState :: (sva{sv}) → ()
 *
 *   Requests the state of an action to be changed to the given value.
 *
 *   The action is named by the first parameter (s).
 *
 *   The requested new state is given in the second parameter (v).
 *   It must be equal in type to the existing state.
 *
 *   The final parameter (a{sv}) is a list of "platform data".
 *
 *   This method is not guaranteed to have any particular effect.
 *   The implementation of an action can choose to ignore the requested
 *   state change, or choose to change its state to something else or
 *   to trigger other side effects. In particular, callers should expect
 *   their request to be completely ignored when the enabled flag is
 *   false (but even this is not guaranteed).
 *
 * Signals
 * -------
 *
 * Changed :: (asa{sb}a{sv}a{s(bgav)})
 *
 *   Signals that some change has occured to the action group.
 *
 *   Four separate types of changes are possible, and the 4 parameters
 *   of the change signal reflect these possibilities:
 *   as: a list of removed actions
 *   a{sb}: a list of actions that had their enabled flag changed
 *   a{sv}: a list of actions that had their state changed
 *   a{s(bgav)}: a list of new actions added in the same format as
 *               the return value of the DescribeAll method
 */

/* Using XML saves us dozens of relocations vs. using the introspection
 * structure types.  We only need to burn cycles and memory if we
 * actually use the exporter -- not in every single app using GIO.
 *
 * It's also a lot easier to read. :)
 */
const char org_gtk_Actions_xml[] =
  "<node>"
  "  <interface name='org.gtk.Actions'>"
  "    <method name='List'>"
  "      <arg type='as' name='list' direction='out'/>"
  "    </method>"
  "    <method name='Describe'>"
  "      <arg type='s' name='action_name' direction='in'/>"
  "      <arg type='(bgav)' name='description' direction='out'/>"
  "    </method>"
  "    <method name='DescribeAll'>"
  "      <arg type='a{s(bgav)}' name='descriptions' direction='out'/>"
  "    </method>"
  "    <method name='Activate'>"
  "      <arg type='s' name='action_name' direction='in'/>"
  "      <arg type='av' name='parameter' direction='in'/>"
  "      <arg type='a{sv}' name='platform_data' direction='in'/>"
  "    </method>"
  "    <method name='SetState'>"
  "      <arg type='s' name='action_name' direction='in'/>"
  "      <arg type='v' name='value' direction='in'/>"
  "      <arg type='a{sv}' name='platform_data' direction='in'/>"
  "    </method>"
  "    <signal name='Changed'>"
  "      <arg type='as' name='removals'/>"
  "      <arg type='a{sb}' name='enable_changes'/>"
  "      <arg type='a{sv}' name='state_changes'/>"
  "      <arg type='a{s(bgav)}' name='additions'/>"
  "    </signal>"
  "  </interface>"
  "</node>";

static GDBusInterfaceInfo *org_gtk_Actions;
static GHashTable *exported_groups;

typedef struct
{
  GActionGroup    *action_group;
  GDBusConnection *connection;
  gchar           *object_path;
  guint            registration_id;
  GHashTable      *pending_changes;
  guint            pending_id;
  gulong           signal_ids[4];
} GActionGroupExporter;

#define ACTION_ADDED_EVENT             (1u<<0)
#define ACTION_REMOVED_EVENT           (1u<<1)
#define ACTION_STATE_CHANGED_EVENT     (1u<<2)
#define ACTION_ENABLED_CHANGED_EVENT   (1u<<3)

static gboolean
g_action_group_exporter_dispatch_events (gpointer user_data)
{
  GActionGroupExporter *exporter = user_data;
  GVariantBuilder removes;
  GVariantBuilder enabled_changes;
  GVariantBuilder state_changes;
  GVariantBuilder adds;
  GHashTableIter iter;
  gpointer value;
  gpointer key;

  g_variant_builder_init (&removes, G_VARIANT_TYPE_STRING_ARRAY);
  g_variant_builder_init (&enabled_changes, G_VARIANT_TYPE ("a{sb}"));
  g_variant_builder_init (&state_changes, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_init (&adds, G_VARIANT_TYPE ("a{s(bgav)}"));

  g_hash_table_iter_init (&iter, exporter->pending_changes);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      guint events = GPOINTER_TO_INT (value);
      const gchar *name = key;

      /* Adds and removes are incompatible with enabled or state
       * changes, but we must report at least one event.
       */
      g_assert (((events & (ACTION_ENABLED_CHANGED_EVENT | ACTION_STATE_CHANGED_EVENT)) == 0) !=
                ((events & (ACTION_REMOVED_EVENT | ACTION_ADDED_EVENT)) == 0));

      if (events & ACTION_REMOVED_EVENT)
        g_variant_builder_add (&removes, "s", name);

      if (events & ACTION_ENABLED_CHANGED_EVENT)
        {
          gboolean enabled;

          enabled = g_action_group_get_action_enabled (exporter->action_group, name);
          g_variant_builder_add (&enabled_changes, "{sb}", name, enabled);
        }

      if (events & ACTION_STATE_CHANGED_EVENT)
        {
          GVariant *state;

          state = g_action_group_get_action_state (exporter->action_group, name);
          g_variant_builder_add (&state_changes, "{sv}", name, state);
          g_variant_unref (state);
        }

      if (events & ACTION_ADDED_EVENT)
        {
          GVariant *description;

          description = g_action_group_describe_action (exporter->action_group, name);
          g_variant_builder_add (&adds, "{s@(bgav)}", name, description);
        }
    }

  g_hash_table_remove_all (exporter->pending_changes);

  g_dbus_connection_emit_signal (exporter->connection, NULL, exporter->object_path,
                                 "org.gtk.Actions", "Changed",
                                 g_variant_new ("(asa{sb}a{sv}a{s(bgav)})",
                                                &removes, &enabled_changes,
                                                &state_changes, &adds),
                                 NULL);

  exporter->pending_id = 0;

  return FALSE;
}

static guint
g_action_group_exporter_get_events (GActionGroupExporter *exporter,
                                    const gchar          *name)
{
  return (gsize) g_hash_table_lookup (exporter->pending_changes, name);
}

static void
g_action_group_exporter_set_events (GActionGroupExporter *exporter,
                                    const gchar          *name,
                                    guint                 events)
{
  gboolean have_events;
  gboolean is_queued;

  if (events != 0)
    g_hash_table_insert (exporter->pending_changes, g_strdup (name), GINT_TO_POINTER (events));
  else
    g_hash_table_remove (exporter->pending_changes, name);

  have_events = g_hash_table_size (exporter->pending_changes) > 0;
  is_queued = exporter->pending_id > 0;

  if (have_events && !is_queued)
    exporter->pending_id = g_idle_add (g_action_group_exporter_dispatch_events, exporter);

  if (!have_events && is_queued)
    {
      g_source_remove (exporter->pending_id);
      exporter->pending_id = 0;
    }
}

static void
g_action_group_exporter_action_added (GActionGroup *action_group,
                                      const gchar  *action_name,
                                      gpointer      user_data)
{
  GActionGroupExporter *exporter = user_data;
  guint event_mask;

  event_mask = g_action_group_exporter_get_events (exporter, action_name);

  /* The action is new, so we should not have any pending
   * enabled-changed or state-changed signals for it.
   */
  g_assert (~event_mask & (ACTION_STATE_CHANGED_EVENT |
                           ACTION_ENABLED_CHANGED_EVENT));

  event_mask |= ACTION_ADDED_EVENT;

  g_action_group_exporter_set_events (exporter, action_name, event_mask);
}

static void
g_action_group_exporter_action_removed (GActionGroup *action_group,
                                        const gchar  *action_name,
                                        gpointer      user_data)
{
  GActionGroupExporter *exporter = user_data;
  guint event_mask;

  event_mask = g_action_group_exporter_get_events (exporter, action_name);

  /* If the add event for this is still queued then just cancel it since
   * it's gone now.
   *
   * If the event was freshly added, there should not have been any
   * enabled or state changed events.
   */
  if (event_mask & ACTION_ADDED_EVENT)
    {
      g_assert (~event_mask & ~(ACTION_STATE_CHANGED_EVENT | ACTION_ENABLED_CHANGED_EVENT));
      event_mask &= ~ACTION_ADDED_EVENT;
    }

  /* Otherwise, queue a remove event.  Drop any state or enabled changes
   * that were queued before the remove. */
  else
    {
      event_mask |= ACTION_REMOVED_EVENT;
      event_mask &= ~(ACTION_STATE_CHANGED_EVENT | ACTION_ENABLED_CHANGED_EVENT);
    }

  g_action_group_exporter_set_events (exporter, action_name, event_mask);
}

static void
g_action_group_exporter_action_state_changed (GActionGroup *action_group,
                                              const gchar  *action_name,
                                              GVariant     *value,
                                              gpointer      user_data)
{
  GActionGroupExporter *exporter = user_data;
  guint event_mask;

  event_mask = g_action_group_exporter_get_events (exporter, action_name);

  /* If it was removed, it must have been added back.  Otherwise, why
   * are we hearing about changes?
   */
  g_assert (~event_mask & ACTION_REMOVED_EVENT ||
            event_mask & ACTION_ADDED_EVENT);

  /* If it is freshly added, don't also bother with the state change
   * signal since the updated state will be sent as part of the pending
   * add message.
   */
  if (~event_mask & ACTION_ADDED_EVENT)
    event_mask |= ACTION_STATE_CHANGED_EVENT;

  g_action_group_exporter_set_events (exporter, action_name, event_mask);
}

static void
g_action_group_exporter_action_enabled_changed (GActionGroup *action_group,
                                                const gchar  *action_name,
                                                gboolean      enabled,
                                                gpointer      user_data)
{
  GActionGroupExporter *exporter = user_data;
  guint event_mask;

  event_mask = g_action_group_exporter_get_events (exporter, action_name);

  /* Reasoning as above. */
  g_assert (~event_mask & ACTION_REMOVED_EVENT ||
            event_mask & ACTION_ADDED_EVENT);

  if (~event_mask & ACTION_ADDED_EVENT)
    event_mask |= ACTION_ENABLED_CHANGED_EVENT;

  g_action_group_exporter_set_events (exporter, action_name, event_mask);
}

static void
g_action_group_exporter_pre_emit (GActionGroupExporter *exporter,
                                  GVariant             *platform_data)
{
  if (G_IS_APPLICATION (exporter->action_group))
    G_APPLICATION_GET_CLASS (exporter->action_group)
      ->before_emit (G_APPLICATION (exporter->action_group), platform_data);
}

static void
g_action_group_exporter_post_emit (GActionGroupExporter *exporter,
                                   GVariant             *platform_data)
{
  if (G_IS_APPLICATION (exporter->action_group))
    G_APPLICATION_GET_CLASS (exporter->action_group)
      ->after_emit (G_APPLICATION (exporter->action_group), platform_data);
}

static void
org_gtk_Actions_method_call (GDBusConnection       *connection,
                             const gchar           *sender,
                             const gchar           *object_path,
                             const gchar           *interface_name,
                             const gchar           *method_name,
                             GVariant              *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer               user_data)
{
  GActionGroupExporter *exporter = user_data;
  GVariant *result = NULL;

  if (g_str_equal (method_name, "List"))
    {
      gchar **list;

      list = g_action_group_list_actions (exporter->action_group);
      result = g_variant_new ("(^as)", list);
      g_strfreev (list);
    }

  else if (g_str_equal (method_name, "Describe"))
    {
      const gchar *name;
      GVariant *desc;

      g_variant_get (parameters, "(&s)", &name);
      desc = g_action_group_describe_action (exporter->action_group, name);
      result = g_variant_new ("(@(bgav))", desc);
    }

  else if (g_str_equal (method_name, "DescribeAll"))
    {
      GVariantBuilder builder;
      gchar **list;
      gint i;

      list = g_action_group_list_actions (exporter->action_group);
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{s(bgav)}"));
      for (i = 0; list[i]; i++)
        {
          const gchar *name = list[i];
          GVariant *description;

          description = g_action_group_describe_action (exporter->action_group, name);
          g_variant_builder_add (&builder, "{s@(bgav)}", name, description);
        }
      result = g_variant_new ("(a{s(bgav)})", &builder);
      g_strfreev (list);
    }

  else if (g_str_equal (method_name, "Activate"))
    {
      GVariant *parameter = NULL;
      GVariant *platform_data;
      GVariantIter *iter;
      const gchar *name;

      g_variant_get (parameters, "(&sav@a{sv})", &name, &iter, &platform_data);
      g_variant_iter_next (iter, "v", &parameter);
      g_variant_iter_free (iter);

      g_action_group_exporter_pre_emit (exporter, platform_data);
      g_action_group_activate_action (exporter->action_group, name, parameter);
      g_action_group_exporter_post_emit (exporter, platform_data);

      if (parameter)
        g_variant_unref (parameter);

      g_variant_unref (platform_data);
    }

  else if (g_str_equal (method_name, "SetState"))
    {
      GVariant *platform_data;
      const gchar *name;
      GVariant *state;

      g_variant_get (parameters, "(&sv@a{sv})", &name, &state, &platform_data);
      g_action_group_exporter_pre_emit (exporter, platform_data);
      g_action_group_change_action_state (exporter->action_group, name, state);
      g_action_group_exporter_post_emit (exporter, platform_data);
      g_variant_unref (platform_data);
      g_variant_unref (state);
    }

  else
    g_assert_not_reached ();

  g_dbus_method_invocation_return_value (invocation, result);
}

/**
 * g_action_group_dbus_export_start:
 * @connection: a #GDBusConnection
 * @object_path: a D-Bus object path
 * @action_group: a #GActionGroup
 * @error: a pointer to a %NULL #GError, or %NULL
 *
 * Exports @action_group on @connection at @object_path.
 *
 * The implemented D-Bus API should be considered private.  It is
 * subject to change in the future.
 *
 * A given action group can only be exported on one object path and an
 * object path can only have one action group exported on it. If either
 * constraint is violated, the export will fail and %FALSE will be
 * returned (with @error set accordingly).
 *
 * Use g_action_group_dbus_export_stop() to stop exporting @action_group,
 * or g_action_group_dbus_export_query() to find out if and where a given
 * action group is exported.
 *
 * Returns: %TRUE if the export is successful, or %FALSE (with @error
 *          set) in the event of a failure.
 **/
gboolean
g_action_group_dbus_export_start (GDBusConnection  *connection,
                                  const gchar      *object_path,
                                  GActionGroup     *action_group,
                                  GError          **error)
{
  const GDBusInterfaceVTable vtable = {
    org_gtk_Actions_method_call
  };
  GActionGroupExporter *exporter;

  if G_UNLIKELY (exported_groups == NULL)
    exported_groups = g_hash_table_new (NULL, NULL);

  if G_UNLIKELY (org_gtk_Actions == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml (org_gtk_Actions_xml, &error);
      if G_UNLIKELY (info == NULL)
        g_error ("%s", error->message);
      org_gtk_Actions = g_dbus_node_info_lookup_interface (info, "org.gtk.Actions");
      g_assert (org_gtk_Actions != NULL);
      g_dbus_interface_info_ref (org_gtk_Actions);
      g_dbus_node_info_unref (info);
    }

  if G_UNLIKELY (g_hash_table_lookup (exported_groups, action_group))
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_EXISTS,
                   "The given GActionGroup has already been exported");
      return FALSE;
    }

  exporter = g_slice_new (GActionGroupExporter);
  exporter->registration_id = g_dbus_connection_register_object (connection, object_path, org_gtk_Actions,
                                                                 &vtable, exporter, NULL, error);

  if (exporter->registration_id == 0)
    {
      g_slice_free (GActionGroupExporter, exporter);
      return FALSE;
    }

  g_hash_table_insert (exported_groups, action_group, exporter);
  exporter->pending_changes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  exporter->pending_id = 0;
  exporter->action_group = g_object_ref (action_group);
  exporter->connection = g_object_ref (connection);
  exporter->object_path = g_strdup (object_path);

  exporter->signal_ids[0] =
    g_signal_connect (action_group, "action-added",
                      G_CALLBACK (g_action_group_exporter_action_added), exporter);
  exporter->signal_ids[1] =
    g_signal_connect (action_group, "action-removed",
                      G_CALLBACK (g_action_group_exporter_action_removed), exporter);
  exporter->signal_ids[2] =
    g_signal_connect (action_group, "action-state-changed",
                      G_CALLBACK (g_action_group_exporter_action_state_changed), exporter);
  exporter->signal_ids[3] =
    g_signal_connect (action_group, "action-enabled-changed",
                      G_CALLBACK (g_action_group_exporter_action_enabled_changed), exporter);

  return TRUE;
}

/**
 * g_action_group_dbus_export_stop:
 * @action_group: a #GActionGroup
 *
 * Stops the export of @action_group.
 *
 * This reverses the effect of a previous call to
 * g_action_group_dbus_export_start() for @action_group.
 *
 * Returns: %TRUE if an export was stopped or %FALSE if @action_group
 *          was not exported in the first place
 **/
gboolean
g_action_group_dbus_export_stop (GActionGroup *action_group)
{
  GActionGroupExporter *exporter;
  gint i;

  if G_UNLIKELY (exported_groups == NULL)
    return FALSE;

  exporter = g_hash_table_lookup (exported_groups, action_group);
  if G_UNLIKELY (exporter == NULL)
    return FALSE;

  g_hash_table_remove (exported_groups, action_group);

  g_dbus_connection_unregister_object (exporter->connection, exporter->registration_id);
  for (i = 0; i < G_N_ELEMENTS (exporter->signal_ids); i++)
    g_signal_handler_disconnect (exporter->action_group, exporter->signal_ids[i]);
  g_object_unref (exporter->connection);
  g_object_unref (exporter->action_group);
  g_free (exporter->object_path);

  g_slice_free (GActionGroupExporter, exporter);

  return TRUE;
}

/**
 * g_action_group_dbus_export_query:
 * @action_group: a #GActionGroup
 * @connection: (out): the #GDBusConnection used for exporting
 * @object_path: (out): the object path used for exporting
 *
 * Queries if and where @action_group is exported.
 *
 * If @action_group is exported, %TRUE is returned.  If @connection is
 * non-%NULL then it is set to the #GDBusConnection used for the export.
 * If @object_path is non-%NULL then it is set to the object path.
 *
 * If the @action_group is not exported, %FALSE is returned and
 * @connection and @object_path remain unmodified.
 *
 * Returns: %TRUE if @action_group was exported, else %FALSE
 **/
gboolean
g_action_group_dbus_export_query (GActionGroup     *action_group,
                                  GDBusConnection **connection,
                                  const gchar     **object_path)
{
  GActionGroupExporter *exporter;

  if (exported_groups == NULL)
    return FALSE;

  exporter = g_hash_table_lookup (exported_groups, action_group);
  if (exporter == NULL)
    return FALSE;

  if (connection)
    *connection = exporter->connection;

  if (object_path)
    *object_path = exporter->object_path;

  return TRUE;
}